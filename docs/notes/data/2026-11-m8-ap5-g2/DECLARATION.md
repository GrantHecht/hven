# M8 AP-5 G2 -- DECLARATION (recovery-aware stop; committed before any leg; never edited afterwards)

## Spec and change
Design page docs/notes/2026-11-m8-ap5-interface-design.md, row G2: the restoration-entry stash on both engines, the token
forwarded into the SQP restoration sub-solve, `IterationEvent::recovering`, a token stop inside restoration returns the
restoration-entry point (message gains "; returned the point before feasibility restoration"), a callback kStop on a
`recovering` IPM event returns the shown point; the two SQP restoration-stop tests re-derived; T5, T9, T14; the
ipm_solver.h / ipm_solver_impl.h "restoration is silent" text corrected. Declared stop path only: no corpus cell stops, so
the replay is byte-exact, plus the ARMED leg (callback installed, never-fired token).

## Code head and base
Code head 99cc2d2ac (3 commits: d9286a6fb feature, fcd566177 bench-qualification fix + warm-start pin, 99cc2d2ac T5A
convergence-row pin) rebased on origin/m8 286fb5398 (walk-cap fix landed there; both behaviours kept: its kRestore on an
elastic zero step at h > feas_tol, G2's token handling; the rebase merged textually clean and the walk-cap site's
`mj.token_exit = st.token_interrupt` pattern is present at all four restoration-return sites). Arm A = origin/m8 286fb5398
(NOT 92e7c5d83: the walk-cap fix is the base's, so it must not appear as a G2 difference). Source/include/tests/bench/
examples/cmake/scripts at the chain's guard are identical to 99cc2d2ac.

## Deviations from the page, recorded
D-a  IPM stash is larger than the page's "five members": it also holds the iteration row (Citer), mu, and the RHS is stored
     in iteration-top form -- every post-factorization door sees the primal block in mu-form (NEWTON FORM IN), so the stash
     undoes it from bound_grad_scratch_ (the stall door, pre-factorization, does not need to). A new
     `solve_exit_stash_point_` keeps the stash point's g/diagnostics from being emptied by solve_exit_restoration_active_.
     Orchestrator ruling: ACCEPTED, correctness requires it.
D-b  SQP: parent state is untouched until the sub-solve returns, so the "stash" is x/lambda/at_start copied at
     enter_restoration (armed only) and restored; a stopped restoration phase NEVER resumes (a stop is a stop); its verdict
     is still computed (a certificate it earned is kept). Orchestrator ruling: ACCEPTED; migration note kept.
D-c  Warm start of an IPM stash exit: the page said "kSeeded resolution"; the code captures the entry point's x and equality
     multipliers CORE ONLY, with NO polish extension (restoration-flagged exit capture), pinned by a test (T5A). The
     migration-0.2.md row says exactly that. Resolution level is whatever the core-only payload resolves to on re-entry;
     the page's literal "kSeeded" is not asserted.
D-d  The IPM stall door is reached only through the tests/interior make_stall_solver fixture (one recovering iteration there).
D-e  Bench lever: HVEN_LEG_UNFIRED_TOKEN (bench/corpus_cells.h corpus_leg_token; bench/ipm_corpus_leg.cpp) -- unset, the
     budget is byte-for-byte what it was.
D-f  dep/eigen, dep/fmt in this worktree are plain copies of the main checkout's submodule trees (submodule update refused).

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release tree build-g2/rel and Debug build-g2/dbg, every target, built and ctest'ed on this code head: 100% of 3665 in
both. The first full build (pre-rebase) caught a bench compile error (unqualified corpus_leg_token), fixed in fcd566177.
Mutation checks (each applied locally, the two driver/interior test binaries re-run, file restored; run on the rebased head):
  a ipm skip stash restore            killed (3: G2A stall door, G2A soft-step door, T5A)
  b ipm drop Newton-form undo         killed (T5A)
  c ipm drop stash-point un-contam.   killed (T5A)
  d sqp drop never-resume gate        SURVIVES (resumed phase hits the latch at its first poll and ends at the entry point;
                                       the gate is defence in depth on every existing fixture; recorded, not pinned)
  e sqp drop return_entry_point       killed (4 SQP restoration-stop tests)
  f sqp do not forward the token      killed (2)
  g sqp drop G1 SOC undo              killed (ATokenStopOnASocPromotedRow...)
  h sqp drop token_interrupt label    killed (2)
  i sqp drop G1 failure-step undo     SURVIVES (no small public fixture reaches qp_failure_steps_accepted on a post-push
                                       stop; recorded, pinned by mutation only per ruling)
  j ipm recovering flag always false  killed (6)
  k ipm stash keeps no mu             SURVIVES (mu is not observable at the result boundary; recorded)
  l ipm stash keeps no iteration row  killed (T5A convergence-row pin, added after the first round showed it surviving)

## Tests (P1 count)
ctest, as it counts them: 3665 run, Release and Debug (`ctest -N` lists 3667: the 2 Disabled entries
EqpRefinementAb.FootprintRuleProbe/FullBattery are listed but not run; the Skipped FailByDesignControl entry is inside
the 3665 as ctest reports "tests passed ... out of 3665"). 3653 (origin/m8 286fb5398) + 12 new TESTs, verified from the
diff: test_stop_recovery.cpp 5, test_callback.cpp 5 (4 SQP token/restoration + the SOC-undo pin), test_ipm_stop_reason.cpp 2.
The two re-derived SQP restoration-stop tests are existing TESTs modified in place.

## Binaries and trees (git-ignored build-g2/)
Arm A  build-g2/armAbin/hven_sqp_corpus  built by legs/prebuild.sh from `git archive 286fb5398` (+ dep copies)
       sha256 f9ac57700881c4c34fc70fc50865e08cf89eb798fe445e50b6351bcd2e53868b
Arm B  build-g2/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g2/rel/bench/hven_sqp_corpus
       sha256 e49f15c491019ad0610b30b9fa84c28466ff9c71acc79f55fe47a212b45ea8f5
Trees  build-g2/rel (Release, every target), build-g2/dbg (Debug, every target), build-g2/tsan, build-g2/smoke-* (legs).
Instruments  legs/ and runtime/ in this directory: copies of G1's legs-r3/runtime-r3 re-pointed to build-g2/ and this
directory; replay.sh adds the ARMED leg (`--callback count` + HVEN_LEG_UNFIRED_TOKEN=1, arm B vs arm B, EXACT);
interior.sh adds unmoved / promoted / armed comparisons; tsan.sh filter extended to
StopToken.*:StopTokenSolve.T2*:StopTokenSolve.T10*:StopRecovery.*:Callback.SqpToken* with the clang C compiler as in G1 r2.

## Proofs and pass criteria (each its own script under `flock /tmp/box-build.lock`; `pgrep -a hven_bench_cell` empty first;
the runtime gate FIRST and alone, then legs/chain.sh 99cc2d2ac e49f15c4...)
P0  arm A / arm B sha256 above (checked by runtime/run_all.sh, legs/replay.sh, legs/interior.sh).
P1  ctest Release (build-g2/rel) and Debug (build-g2/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3665".
P2  runtime gate (runtime/run_all.sh <armB> <armA>): interior, ipm, ssn, walk; THROTTLE == 0 or VOID; VERDICT PASS (counter
    identity per mode; filtered B/A in [0.995, 1.005]).
P3  replay 27 cells x (walk, ssn, ipm), arm A vs arm B, compare_replay.py EXACT, 0 differences each; AND the ARMED leg:
    arm B armed (callback + never-fired token) vs arm B unarmed, EXACT, 0 differences x3.
P4  interior: unmoved (A vs B) EXACT 0; promoted (B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv)
    EXACT 0; armed (B armed vs B) EXACT 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass (TU count reported).
P6  the G2 tests (StopRecovery.*, Callback.Sqp*Token*/Sqp*Restoration*, IpmStopReason.G2*) inside P1 in both builds.
P8  ThreadSanitizer (legs/tsan.sh): exit 0 and 0 "WARNING: ThreadSanitizer" lines on the filter above.
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.
