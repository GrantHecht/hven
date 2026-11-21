# M8 AP-5 G2 -- DECLARATION-r3 (settler ruling C346 riders; committed before any leg; never edited afterwards)

## Why a successor
Review of the r2 code head (fe39616dd) found it fit to land with riders; the settler ruled C346 R1-R9 and asked for them as
new commits on g2 (the r2 commits are not rewritten). The riders change code (one behaviour change, one new result field, a
narrower stash arming) and tests, so the r2 evidence does not cover the new head. DECLARATION.md and DECLARATION-r2.md and the r2
evidence are untouched; r2's criteria carry over except where this file says otherwise.

## The change (code head 3d6932485 over fe39616dd, on origin/m8 7bb635605)
R1  SQP enter_restoration: `stopped_in_recovery` is `rs.status == kInterrupted` (the restoration sub-solve was cut short), no longer the
    parent latch. A stop on the sub-solve's CONVERGED terminal row leaves its verdict alone: on the exits the latch is cleared, so the
    certified exit stays kInfeasible at x_r with infeasibility_certified (the callback now agrees with the token, which already did);
    when the converged phase resumes, the latch ends the solve at the restored point at the next boundary. solve_result.h's
    infeasibility_certified doc says it is never set on an interrupted exit.
R2  test SqpStopOnAMidPhaseRowWhosePointIsAlreadyFeasibleNeverResumes (kills the never-resume-gate mutation d).
R3  the two converged-row tests re-derived, unconditional, renamed (...IsANoOpAndKeepsTheCertifiedExit); plus the resuming-phase test.
R4  SolveResult::returned_pre_restoration_point on both engines (message kept); tests use it; docs/migration-0.2.md.
R6  IPM stall-door and soft-step-door pins: g.size() > 0, non-NaN feasibility, lambda == the last non-recovering event's.
R7  IterationEvent::recovering doc: the stall-door stash point is the last non-recovering event's.
R8  stash armed only when something can consume it: IPM `stop_token_.stop_possible()` alone; SQP a token that can fire or a callback
    forwarded into the sub-solve (kIncludeRestoration).
R9  SolveBudget::stop and SqpSolver::set_iteration_callback docs trimmed to contract; mechanics in docs/migration-0.2.md.
NOT done: the IPM-vs-SQP difference on a callback kStop during a recovering event (registered for the API revisit; recorded in
migration-0.2.md).

## Tests (P1 count)
ctest as it counts them: "100% tests passed, 0 tests failed out of 3685", Release and Debug (`ctest -N` lists 3687; the 2 Disabled
EqpRefinementAb entries are listed, not run). r2: 3683. +2 verified from the diff over fe39616dd (`git diff fe39616dd -- tests | grep
'^[+-]TEST'`): two TESTs added (SqpStopOnTheRestorationsConvergedRowOfAResumingPhaseReturnsTheRestoredPoint,
SqpStopOnAMidPhaseRowWhosePointIsAlreadyFeasibleNeverResumes); the two converged-row TESTs are renames of existing ones.

## P2 (runtime gate) is NOT required, and why
The unarmed per-iteration hot path is untouched: the diff changes enter_restoration (once per restoration entry, and its exits),
dispatch_restoration_entry (once per IPM restoration entry; its arming test got narrower, never wider), and one bool store per IPM
phase at result assembly. Nothing in an iteration loop that no restoration reaches moved, and the declared P3/P4 counter identity
below is the check that no unarmed solve changed. (r2's P2 PASS stands for the code it measured.)

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release build (build-g2/rel, every target) and Debug build (build-g2/dbg, every target) of this head, both complete.
- Targeted gtest runs on the Release dev tree: Callback.Sqp*, StopRecovery.*, StopToken*, IpmStopReason.* all pass.
- An exploratory temporary test (env-driven, removed before the commit) printed the StalledValleyModel depth-1 rows under
  restoration_proximity and a shifted start, to find the mid-phase row for R2.
- legs-r3/mutate.py's string edits were dry-run against copies of the two source files (all sixteen apply); no mutant was built.
- No ctest leg, replay, interior, TSan or mutation leg has run.

## Binaries and trees (git-ignored build-g2/)
Arm A  build-g2/armAbin/hven_sqp_corpus  (origin/m8 7bb635605, unchanged since r2)
       sha256 d80ce1bfd2512b7e830d693eefae6b9d4da48c757add091b5e55bab5cfe31334
Arm B3 build-g2/armBbin3/hven_sqp_corpus frozen (cp -p) from build-g2/rel/bench/hven_sqp_corpus at 3d6932485
       sha256 0ef55263b35bebd516805e28cf6a0c1ec2da27ca4319f4b830752019352a8498
Trees  build-g2/rel, build-g2/dbg, build-g2/tsan (incremental), build-g2/mut-src + build-g2/mut (mutation scratch copy and its build).
Instruments  legs-r3/ in this directory (ctest.sh unchanged; replay.sh, interior.sh, tsan.sh re-pointed; mutations.sh, mutate.py,
chain.sh new). Run as `flock /tmp/box-build.lock bash legs-r3/chain.sh 3d6932485728b84e976644aa73649378784e31b3 <arm B3 sha256>`,
pgrep -a hven_bench_cell empty first. Outputs: suites-r3/, replay-r3/, interior-r3/, tsan-r3/, mutation-r3/ here; raw per-run dumps
under the ignored .scratch/ with a MANIFEST (sha256, size, path) committed.

## Proofs and pass criteria
P0  arm A / arm B3 sha256 above (checked by replay.sh and interior.sh).
P1  ctest Release (build-g2/rel) and Debug (build-g2/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3685".
P3  replay, 27 cells x (walk, ssn, ipm): arm B3 vs the COMMITTED r2 arm B rows (replay/g2-<mode>-B.csv), compare_replay.py EXACT,
    "differences: 0" x3; AND the ARMED leg: arm B3 armed (counting callback + never-requested token) vs arm B3 unarmed, EXACT,
    "differences: 0" x3.
P4  interior, 43 cells: unmoved (arm A vs B3) EXACT 0; promoted (B3 vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv)
    EXACT 0; armed (B3 armed vs B3) EXACT 0.
P8  ThreadSanitizer (legs-r3/tsan.sh, build-g2/tsan, hven_drivers_tests, filter 'StopToken*:StopRecovery.*:Callback*', i.e. every stop and
    callback test in the driver binary): test binary exit 0 and 0 "WARNING: ThreadSanitizer" lines.
M   mutation table (legs-r3/mutations.sh, Release scratch copy, hven_drivers_tests + hven_interior_tests run in full): the control
    (unmutated) fails nothing; each of the declared twelve a..l is applied alone: a, b, c, d, e, f, g, h, j, l must each be KILLED (at
    least one failing test); d must now be killed (it survived in r2); i and k may survive and are reported either way. Extras m (keep
    the converged-phase latch), n (old stopped_in_recovery key), o and p (never set the new field, SQP and IPM) are applied too and
    reported; they are informational, not pass criteria.
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.
P5 (install smoke / export contract) is not re-run: the diff adds one field to a public header and no TU, file or export-surface
change; r2's P5 stands for the TU set it measured.
