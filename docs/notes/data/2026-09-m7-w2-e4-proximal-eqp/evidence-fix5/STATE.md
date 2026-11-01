# E4 FIX ROUND 5 — STATE

## Task
Diagnose ACOPR118 regression (optimal -> numerical_error) at hven pin ff02789b (E4),
found by RE-TAKE 9. Worktree branch `e4` @ 84fb520e. READ-ONLY outside .scratch/e4-fix5.

## Progress log
- [2026-09-27] START. Worktree clean at 84fb520e. Beginning orientation.
- [2026-09-27 20:20] LEG 1 DONE: REPRODUCED ACOPR118 numerical_error at pin ff02789b.
  Direct `hven_bench_cell --lane hven-sqp --problem ACOPR118 --options default
  --budget-iters 100 --assert-reference`, cwd hven-bench, MKL_NUM_THREADS=1
  OMP_NUM_THREADS=1, taskset -c 2, under /tmp/box-build.lock via
  .scratch/e4-fix5/leg.sh (PGREP_NO_UNCOVERED_CELL / IN_LOCK_PGREP_EMPTY /
  FOREGROUND_START 20:18:57 / FOREGROUND_END 20:20:18 rc=0).
  ALL 115 side-file counters IDENTICAL, 0 differing; all 9 results fields identical.
  Transcript: .scratch/e4-fix5/logs/repro.log, logs/repro-cmp.txt,
  repro/ACOPR118-pin.out.
- NEXT: instrument. Plan = scratch copy of the worktree sources + a bench copy,
  env-gated diag patch (the E2-D4 recipe: docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/runners/).
- [2026-09-27 20:26] BUILD PIPELINE VALIDATED AND BYTE-EXACT.
  .scratch/e4-fix5/src   = `git archive 84fb520e` + dep/{eigen,fmt} copies
  .scratch/e4-fix5/bench = `git archive` of hven-bench HEAD 8076aaa (tracked only)
  build.sh -> build-rel (Release/clang++/tests OFF/bench OFF/LTO OFF) -> installs to
    .scratch/e4-fix5/bench/.hven/install-ff02789b46a7 -> bench-build hven_bench_cell.
  OBSERVED: libhven.a sha256 57c458e0...b1393 and hven_bench_cell sha256
  78d65a55...aa43 are BOTH byte-identical to hven-bench's pin build. So the
  scratch pipeline IS the pin, and any diag patch's effect is attributable.
  Runs use cwd=/home/ghecht/Projects/hven-bench (for sets/ and CUTEst data) and
  write ONLY into .scratch/e4-fix5 -- hven-bench is never written.
- NEXT: diag patch (env-gated) on .scratch/e4-fix5/src, per the mechanism map.
- [2026-09-27 20:47] DIAGNOSIS ESSENTIALLY COMPLETE from the instrumented run.
  Instrumented binary: .scratch/e4-fix5/bench-build/lanes/hven/hven_bench_cell
  (diag patch = .scratch/e4-fix5/patch_diag.py + patch_diag2.py on .scratch/e4-fix5/src,
  env-gated HVEN_F5_TRACE / HVEN_F5_QPDUMP; NEVER committed).
  Its run is BIT-IDENTICAL to the pin: 0 of 115 counters differ, all results equal.
  Trace: .scratch/e4-fix5/trace/ACOPR118-diag.err (160 lines). Dumps:
  .scratch/e4-fix5/dumps/ACOPR118_e4/qp0..qp8.txt (D4QP v1).
  FINDINGS (OBSERVED):
  * the ONE kInfeasible QP (harness_qp_ledger_infeasible=1) is solve 1 = the
    SOC RE-SOLVE at MAJOR 0, add-exit Farkas certificate at minor 445, cert=1.
    A SOC QP failure leaves the ORIGINAL kReject verdict standing by design
    (sqp_solver.cpp soc_qp_infeasible), which is why elastic=0/restoration=0.
    PRE-EXISTING: dep_infeasible_certificates was 1 at RT8 too.
  * THE CELL DIES ELSEWHERE. Majors 17,18,19,20 each solve a walk-owned QP that
    returns kNumericalError after 2-3 MINORS on an unrepairable kWrong INERTIA
    verdict (F5TERM worst=0 refined=0 runaway=0 verdict=2 rows=9 pins=30), with
    TR shrinking 0.25 -> 0.125 -> 0.0625 -> 0.03125 and NO E4 counter moving
    (eqp_refusals=0 polish=11..14 drift=0 tp=0 vrefine=0). e2route=0 (blk/zm/certrej
    all 0) so the L3 elastic widening does not apply; the ordinary shrink ladder
    runs out at qp_failure_budget 3 and major 20 takes kFinishQpFailure ->
    map_status(kNumericalError) -> NLP numerical_error.
  * the reported point f=129660.5787807497 has violation_l1 = 0.4995 (kkt_primal_eq
    0.0283): it is INFEASIBLE, not a better feasible point.
  NEXT: oracle the dumps (SOC certificate true/false; is the kWrong verdict TRUE,
  i.e. is the face genuinely indefinite).
- [2026-09-27 21:10] ROOT CAUSE PINNED, E4 EXONERATED. All OBSERVED.
  REPLAY (tools: .scratch/e4-fix5/replay + replay-build-{ff02789b46a7,85dc01ac9f53,diag}):
  * dumps qp1..qp8 replay at the E4 pin reproducing the in-cell status AND minors
    AND factorizations exactly (qp0 does not: in-cell 446 minors/hot K0 vs replay 355).
  * qp5..qp8 (the four killing QPs) at the PRE-E4 pin 85dc01ac: status=3, minors
    3/2/3/3, facts 5/5/5/5 -- IDENTICAL to E4. E4 DID NOT CAUSE THE FAILURE.
  * qp0 (the SOC QP): E4 kInfeasible with a certificate; RT8 kNumericalError, no
    certificate. HiGHS phase-1 min_l1_violation = 16.50; Clarabel PrimalInfeasible.
    THE CERTIFICATE IS TRUE. E4 IMPROVED that solve.
  * qp5 with independent_working_set = FALSE: kOptimal, 682 minors, 36 facts,
    f=-0.154376254009, stat 1.77e-14, rows 5.41e-14, compl 4.87e-15. SOLVABLE.
  * oracle on qp5: FEASIBLE (phase-1 min_l1 = 0.0); H has 2 negative eigenvalues
    (min -1.4419e+02); Z'HZ on the WALK's terminal face (9 rows/30 pins, dim 69)
    min eig -6.8122e-02 -> the kWrong verdict is ARITHMETICALLY TRUE.
  MECHANISM (E2 lane, pre-existing): qp5 minor 0, blocker pin 291 at lower,
  alpha 0.5405, moving; dependency test verdict kUndecidable, rho = the NaN
  default (solves=0, screen=0, stall_admit=0) -> ADMITTED AS A DEFERRED ADD;
  minor 1 spent on the recheck (rows stay 9, pins 30); minor 2 reaches the
  terminal disjunct on that face with verdict kWrong; the re-read does not
  overturn it (dep_inertia_rereads=2 flips=0); the post-probe restart is gated
  off (probe_drop_made=0) and -- MEASURED with the gate removed (HVEN_F5_UNGATE)
  -- repair_temporary_vertex RETURNS FALSE, results byte-identical on all four
  dumps. -> kNumericalError at minor 3.
  With E2 OFF the same pass adds a ROW AND a pin (it=1 rows=10 pins=30) and the
  walk grows monotonically to pins=36 by it=7, 682 minors, verdict kOk.
  DRIVER SIDE: the four kNumericalError QPs carry blk_term=0 zmcap=0 certrej=0,
  so e2_termination_to_elastic is FALSE and the elastic tier is never opened;
  the only response is the TR shrink 0.25 -> 0.125 -> 0.0625 -> 0.03125, which
  cannot change a face's curvature; qp_failure_budget 3 is exhausted and major 20
  takes kFinishQpFailure -> numerical_error.
  NOTE: the E2-off LABEL's whole ACOPR118 cell is dnf_wall at BOTH pins, so E2-off
  is a LOCALIZATION, not a remedy.
  DECISION: NO FIX LANDS THIS ROUND. Both defects are pre-existing and outside
  E4's own mechanisms; register with the numbers. Report next.
- [2026-09-27 21:35] CLOSED. NO FIX LANDED, NO COMMIT MADE (by the round's own rule:
  both defects are pre-existing and outside E4's mechanisms -> registered).
  Report: /home/ghecht/Projects/hven/.superpowers/m7-w2-e4-fix5-report.md
  (410 lines, SIGNOFF: M7-W2-E4-FIX5-REPORT).
  Registered: R1 (E2 lane: undecidable deferred add -> indefinite face -> kWrong
  dead end with no recovery), R2 (driver: walk-owned kNumericalError with
  blk/zm/certrej all 0 gets only the TR shrink ladder; map_status also folds
  kMaxIter into numerical_error; the discriminating columns are not exported),
  R3 (E4 cost profile on the STEENBR/network family: 2 cells to the wall,
  ACOPP118 12.1 s -> 246.9 s).
  Both trees verified clean: worktree at 84fb520e, no porcelain output.
  Scratch retained under .scratch/e4-fix5/ (~1.5 GB incl. build trees) for a
  follow-up round; delete build-rel/bench-build/replay-build-* to reclaim.
