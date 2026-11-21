# M8 AP-5 G4 -- DECLARATION (intra-iteration safe points; committed before any leg; never edited afterwards)

## Spec and change
Design page docs/notes/2026-11-m8-ap5-interface-design.md, row G4 (sections 3.2, 3.5, 3.6 T3/T4, 6 Q10): the IPM inertia
ladder polls the stop token between attempts (the iteration is abandoned and its start point returned, with its evaluation
intact); the SQP main subproblem's minor loops (walk `src/qp/qp_engine.cpp`, SSN `src/qp/ssn_engine.cpp`, IPQP main loop
`src/qp/ipqp_engine.cpp`) and its elastic re-solves poll it (the subproblem is abandoned, the stopped-at row for x_k pushed
with `qp_solved == false`, its event fires, x_k returned). No poll in the line search, SOC, extended backtracking or
watchdog; no time-limit poll (Q10). Behaviour: none without a token, none with a token never requested (T9, ARMED legs).

Cost design (unarmed = no token): every engine holds `detail::stop_flag(budget.stop)`, a `const std::atomic<bool> *` that is
null without a token. Each kernel hoists it into a local before its loop; the poll is `detail::poll_stop(stop, site)` =
one null test, `[[likely]]` not taken (`include/hven/core/detail/stop_poll.h`). The IPM passes the pointer to
`factor_impl`, read once per call, tested before each ladder rung. The SQP driver arms the engines once per solve
(`arm_qp_stop`, a few pointer stores), reads `stop_flag_set(qp_stop_)` after each main dispatch and each elastic ladder (one
null test per major), and snapshots the row only when armed. Members are appended last (no existing offset moves).

## Code head and base
Code head 11a984f45d8cc586a13b903e624663c232f987c6 on origin/m8 f1d8a8d19 (G2 landed). Arm A = origin/m8 f1d8a8d19. Source/include/tests/bench/examples/
cmake/scripts at the chain's guard are identical to 11a984f45d8cc586a13b903e624663c232f987c6.

## Where landed code differs from the page (recorded)
D-a  T3's mechanism: the page's "KKT hook calls request_stop() on the 2nd factorization of a ladder" cannot work -- the KKT
     hook and the iteration callback both run BEFORE the iteration-top poll, which sees the request first, and no user code
     runs between that poll and the ladder. T3 and T4 use one new HVEN_TESTING seam instead (StopPollSeam inside
     `detail::poll_stop`, docs/testing.md "An eighth seam"; Apache-2.0 files only, no deviation, no notices/ entry), compiled
     into hven_sqp_seam_tests. It requests the stop through the public StopSource at a chosen armed poll.
D-b  The abandoned kernel leaves through its own iteration-budget exit (walk kIterationLimit; SSN kIterationLimit/kBudget;
     IPQP kBudget), so a stopped SSN/IPQP subproblem is counted (and, under a trace, reported) as a budget escape; the driver
     discards the answer. Recorded in docs/migration-0.2.md. No new QpStatus/escape value.
D-c  The abandoned major's minors and factorizations stay in the aggregates; `major_iters` (and `full_step_majors`) are put
     back, so `history.size() == major_iters + 1` as at the exit test.
D-d  IPM ladder stop inside restoration: the exit is the iteration-top stop's own code (one `honour_stop` lambda, G2's stash
     included), but no fixture reaches a ladder rung inside restoration (16 fixture x restoration-mode x inertia-mode
     combinations probed, disclosed below), so T3's restoration half is by construction, not by a test. The SQP half is
     tested (T4 restoration).
D-e  SOC: the walk is disarmed around the SOC re-solve (not a safe point); pinned by StopPollSeam.TheSocResolveIsNotArmed.
D-f  dep/eigen, dep/fmt are plain copies of the main checkout's submodule trees at the pinned commits (submodule update
     refused in the worktree).

## Pre-declaration runs (NOT evidence, disclosed)
- Dev Release (build-g4/rel) and Debug (build-g4/dbg) trees, every target, built on this code head's content; ctest
  (legs/ctest.sh) "100% tests passed, 0 tests failed out of 3691" in both (`ctest -N` also lists the 2 Disabled EqpRefinementAb
  entries; the Skipped FailByDesignControl entry is inside the 3691).
- StopPollSeam.* developed against the Release seam binary; a temporary probe (removed before the code commit) printed the
  InfeasibleCircleLine trajectory (walk solves per major, the elastic and restoration majors) to place T4's elastic and
  restoration polls, and probed 16 IPM fixtures x restoration modes x inertia modes for a ladder rung inside restoration
  (none; D-d).
- legs/mutate.py dry-applied to copies of the five source files: all eighteen apply exactly once. No mutant was built.
- Arm A built (legs/prebuild.sh f1d8a8d19).
- No runtime, instruction-count, replay, interior, smoke, TSan or mutation leg has run.

## Tests (P1 count)
ctest as it counts them: "100% tests passed, 0 tests failed out of 3691", Release and Debug. G2 r3: 3685 (origin/m8
f1d8a8d19). +6 from the diff (`git diff f1d8a8d19 -- tests | grep '^+TEST'`): tests/sqp/test_stop_poll_seams.cpp
StopPollSeam.{T3ATokenStopBetweenLadderAttemptsReturnsTheIterationsStartPoint, T4ATokenStopInsideTheMainSubproblemReturnsXkOnEachQpMode,
T4ATokenStopInsideAnElasticResolveReturnsXk, T4AStopInsideARestorationSubproblemReturnsTheEntryPoint, TheSocResolveIsNotArmed,
T9AnArmedUnfiredSolveIsTheUnarmedSolveOnEachQpMode}.

## Binaries and trees (git-ignored build-g4/)
Arm A  build-g4/armAbin/hven_sqp_corpus  (legs/prebuild.sh f1d8a8d19)   sha256 1cfd3715ecf26969362592edc50ea07645aa8bf2bbbd2dd5348f4cc592f9e8e9
Arm B  build-g4/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g4/rel/bench/hven_sqp_corpus at 11a984f45d8cc586a13b903e624663c232f987c6   sha256 164b3fd0f41f8caea3c1546e6e27dde71dbc2d62728979151fdb5ea1f5f06d59
Trees  build-g4/rel, build-g4/dbg, build-g4/tsan, build-g4/mut-src + build-g4/mut, build-g4/smoke-*.
Raw dumps under the ignored .scratch/ (g4-runtime, g4-ic, g4-suites, g4-install-smoke, g4-tsan, g4-mutation); MANIFEST.txt
(sha256, size, path) committed with the evidence.

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, foreground)
1. runtime/run_all.sh 164b3fd0f41f8caea3c1546e6e27dde71dbc2d62728979151fdb5ea1f5f06d59 1cfd3715ecf26969362592edc50ea07645aa8bf2bbbd2dd5348f4cc592f9e8e9        -- FIRST, serial, solo (nothing else of mine on the box)
2. ic/run_ic.sh 164b3fd0f41f8caea3c1546e6e27dde71dbc2d62728979151fdb5ea1f5f06d59 1cfd3715ecf26969362592edc50ea07645aa8bf2bbbd2dd5348f4cc592f9e8e9              -- serial, solo
3. legs/chain.sh 11a984f45d8cc586a13b903e624663c232f987c6 164b3fd0f41f8caea3c1546e6e27dde71dbc2d62728979151fdb5ea1f5f06d59        -- ctest x2, replay, interior, install smoke, TSan, mutations

## Proofs and pass criteria
P0  arm A / arm B sha256 above (checked by run_all.sh, run_ic.sh, replay.sh, interior.sh).
P1  ctest Release (build-g4/rel) and Debug (build-g4/dbg), -j4 under setsid: "100% tests passed, 0 tests failed out of 3691".
P2  (the P5 runtime gate of the page; runtime/run_all.sh, G2's instrument): interior, ipm, ssn, walk; per mode COUNTER
    IDENTITY HOLDS, THROTTLE 0 (else VOID), filtered B/A in [0.995, 1.005]; VERDICT PASS.
IC  pass A/B instruction counts (ic/run_ic.sh; AP-2 B1's pass A instrument): walk, ssn, ipm (the three minor loops) and
    interior (the ladder); three rounds, arms alternated per cell, per-cell median, corpus = sum of medians. PASS per mode:
    counter check 0 differences; corpus instructions:u B/A in [0.999, 1.001] (declared threshold, tighter than the page's
    +-0.5 %); every cell in [0.995, 1.005]; IC_VERDICT PASS. Cycles are informational.
P3  replay 27 cells x (walk, ssn, ipm): arm B vs the committed G2 r3 rows (../2026-11-m8-ap5-g2/replay-r3/r3-<mode>-B3.csv),
    compare_replay.py EXACT, "differences: 0" x3; AND the ARMED leg (counting callback + never-requested token, which arms
    every G4 poll): arm B armed vs arm B, EXACT, "differences: 0" x3.
P4  interior 43 cells: unmoved (arm A vs B) EXACT 0; promoted (B vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/
    interior_baseline.csv) EXACT 0; armed (B armed vs B) EXACT 0.
P5  install smoke, default and clang arms, each with scripts/check_export_contract.sh: pass (TU count reported).
P6  T3/T4 (StopPollSeam.*) inside P1 in both builds.
P8  ThreadSanitizer (legs/tsan.sh, hven_drivers_tests, filter 'StopToken*:StopRecovery.*:Callback*'): exit 0, 0 reports.
M   mutation table (legs/mutations.sh; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests run in full): the
    control fails nothing; a, c, d, e, f, g, h, i, j, k, l, m, n, o, r must each be KILLED; b, p, q may survive and are reported either way.
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.

Mutations (legs/mutate.py): a ipm ladder not handed the flag; b ipm ladder stop without re-pushing the event's row; c ipm
ladder stop labelled a callback stop; d/e/f walk/SSN/IPQP poll ticks but the loop continues; g no abandon after the main
subproblem; h no abandon after the elastic ladder; i stopped row not restored to the exit test's row; j abandoned major
still counted; k SOC re-solve left armed; l/m lazy SSN/IPQP engine not armed; n/o SSN/IPQP route runs on after a stop;
p certified fallback runs on after a stop (no fixture stops inside rung A); q the flag not cleared at solve exit (defence:
every solve re-arms at entry); r the abandon does not label the token stop.
